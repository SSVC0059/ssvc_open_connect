#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

// wingdi.h на Windows определяет ERROR как макрос (0) и ломает enum class Status { ..., ERROR }
// Локальная защита для нативной сборки на MinGW; на ESP32/ubuntu-latest CI это не нужно.
#ifdef ERROR
#undef ERROR
#endif
#include "core/SsvcLogProtocol/SsvcLogProtocol.h"

TEST_CASE("GET_LOG requests validate process numbers") {
  std::string request;
  CHECK(SsvcLogProtocol::formatListRequest(request));
  CHECK(request == "GET_LOG\n");
  CHECK(SsvcLogProtocol::formatFileRequest(15, request));
  CHECK(request == "GET_LOG 15\n");
  CHECK_FALSE(SsvcLogProtocol::formatFileRequest(0, request));
}

TEST_CASE("GET_LOG list response is parsed") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginList();

  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"LIST","files":["15.CSV","16.CSV"]})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::LIST_RECEIVED);
  REQUIRE(transfer.files().size() == 2);
  CHECK(transfer.files()[0] == "15.CSV");
  CHECK(transfer.files()[1] == "16.CSV");
}

TEST_CASE("GET_LOG chunks are decoded in order") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);

  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":2})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::RECEIVING);
  CHECK(transfer.consume(R"({"type":"file","chunk":1,"data":"SGVsbG8g"})"));
  CHECK_FALSE(transfer.consume(R"({"type":"file","chunk":1,"data":"V29ybGQ="})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);

  transfer.beginFile(15);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":2})"));
  CHECK(transfer.consume(R"({"type":"file","chunk":1,"data":"SGVsbG8g"})"));
  CHECK(transfer.consume(R"({"type":"file","chunk":2,"data":"V29ybGQ="})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::COMPLETED);
  CHECK(transfer.data() == "Hello World");
  CHECK(transfer.receivedChunks() == 2);
}

TEST_CASE("GET_LOG errors are retained") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginList();
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"err_nf"})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
  CHECK(transfer.error() == "err_nf");
}

TEST_CASE("GET_LOG treats total:0 as an empty journal") {
  // Пустой журнал: чанков не будет, поэтому передача завершается сразу.
  // Раньше такой файл считался повреждённым кадром (invalid_start), и скачать
  // его было нельзя — тест требовал именно отказа и разошёлся с прошивкой.
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":0})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::COMPLETED);
  CHECK(transfer.data().empty());
}

TEST_CASE("cancel stops the file session and drains") {
  // Отмена (STOP, смена файла) переводит передачу в дренирование: слот ещё занят
  // кадрами старого потока, новые запросы не пускаются. Потерянный хвост не должен
  // держать её вечно — deadline обязан снять блокировку.
  SsvcLogProtocol::Transfer transfer;
  CHECK(transfer.beginFile(15, 1000));
  CHECK(transfer.isBlocked());

  transfer.cancel();

  CHECK(transfer.isBlocked());
  CHECK(transfer.checkTimeout(2000, 1000));
  CHECK_FALSE(transfer.isBlocked());
}

TEST_CASE("WAITING_LIST expires by timeout") {
  // LIST/START могли потеряться: без deadline сессия оставалась бы занятой
  // навсегда и блокировала лог-эндпоинты.
  SsvcLogProtocol::Transfer transfer;
  CHECK(transfer.beginList(1000));
  CHECK(transfer.isBlocked());
  CHECK_FALSE(transfer.checkTimeout(1500, 1000));
  CHECK(transfer.checkTimeout(2500, 1000));
  CHECK_FALSE(transfer.isBlocked());
  CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
  CHECK(transfer.error() == "timeout");
}

TEST_CASE("GET_LOG stops at the total size limit") {
  // Журнал ограничен LOG_TOTAL_RAW_MAX: превышение отвергается, а не растит
  // std::string до OOM.
  SsvcLogProtocol::Transfer transfer;
  CHECK(transfer.beginFile(15, 1000));

  // Чанк 600 байт (FILE_CHUNK_RAW_MAX): 109 чанков ещё в лимите 64 КБ, 110-й — нет.
  constexpr int kChunks = 110;
  CHECK(transfer.consume(R"({"type":"response","request":"GET_LOG",)"
                         R"("result":"START","file":"15.CSV","total":110})"));

  const std::string chunk(800, 'A');
  for (int i = 1; i < kChunks; ++i) {
    const std::string frame = R"({"type":"file","chunk":)" + std::to_string(i) +
                              R"(,"data":")" + chunk + R"("})";
    CHECK(transfer.consume(frame.c_str()));
  }

  const std::string overflow = R"({"type":"file","chunk":)" + std::to_string(kChunks) +
                               R"(,"data":")" + chunk + R"("})";
  CHECK_FALSE(transfer.consume(overflow.c_str()));
  CHECK(transfer.error() == "log_too_large");
}

TEST_CASE("GET_LOG rejects malformed transfer packets") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);
  // Отрицательное число чанков — повреждённый кадр.
  CHECK_FALSE(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":-1})"));
  CHECK(transfer.error() == "invalid_start");

  transfer.beginFile(15);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":1})"));
  CHECK_FALSE(transfer.consume(
      R"({"type":"file","chunk":1,"data":"A==="})"));
  CHECK(transfer.error() == "invalid_data");
}

TEST_CASE("GET_LOG enforces chunk sequence") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":2})"));
  CHECK_FALSE(transfer.consume(
      R"({"type":"file","chunk":2,"data":"SGVsbG8="})"));
  CHECK(transfer.error() == "invalid_chunk");
}

TEST_CASE("GET_LOG error codes are propagated") {
  SsvcLogProtocol::Transfer transfer;
  SUBCASE("err_num") {
    transfer.beginList();
    transfer.consume(
        R"({"type":"response","request":"GET_LOG","result":"err_num"})");
    CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
    CHECK(transfer.error() == "err_num");
  }
  SUBCASE("err_mnt") {
    transfer.beginList();
    transfer.consume(
        R"({"type":"response","request":"GET_LOG","result":"err_mnt"})");
    CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
    CHECK(transfer.error() == "err_mnt");
  }
  SUBCASE("err_int") {
    transfer.beginList();
    transfer.consume(
        R"({"type":"response","request":"GET_LOG","result":"err_int"})");
    CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
    CHECK(transfer.error() == "err_int");
  }
}

TEST_CASE("GET_LOG accepts an empty chunk and completes") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":1})"));
  CHECK(transfer.consume(R"({"type":"file","chunk":1,"data":""})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::COMPLETED);
  CHECK(transfer.data().empty());
  CHECK(transfer.receivedChunks() == 1);
}

TEST_CASE("GET_LOG rejects chunk exceeding FILE_CHUNK_RAW_MAX") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":1})"));
  // 601 нулевой байт → 200 групп "AAAA" (800 симв.) + "AA==" (4 симв.) = 804 символа base64.
  const std::string oversized = std::string(800, 'A') + "AA==";
  const std::string payload =
      std::string(R"({"type":"file","chunk":1,"data":")") + oversized + "\"}";
  CHECK_FALSE(transfer.consume(payload.c_str()));
  CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
  CHECK(transfer.error() == "invalid_data");
}

TEST_CASE("GET_LOG detects stalled transfer by timeout") {
  SsvcLogProtocol::Transfer transfer;
  // В IDLE таймаут не активен.
  CHECK_FALSE(transfer.checkTimeout(5000));

  // beginList без метки времени — отслеживание не включается.
  transfer.beginList();
  CHECK_FALSE(transfer.checkTimeout(5000));

  // beginFile фиксирует время старта — таймаут начинает отсчёт.
  transfer.beginFile(15, 1000);
  CHECK(transfer.status() == SsvcLogProtocol::Status::RECEIVING);
  CHECK(transfer.lastActivityMs() == 1000);

  CHECK_FALSE(transfer.checkTimeout(1500));    // 500 < 10000
  CHECK_FALSE(transfer.checkTimeout(10999));   // 9999 < 10000
  CHECK(transfer.checkTimeout(11000));         // 10000 >= 10000 → таймаут
  CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
  CHECK(transfer.error() == "timeout");
}

TEST_CASE("GET_LOG updates activity timestamp on each consume") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15, 1000);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":2})",
      3000));
  CHECK(transfer.lastActivityMs() == 3000);
  CHECK_FALSE(transfer.checkTimeout(12999));  // 9999 < 10000
  CHECK(transfer.checkTimeout(13000));        // 10000 >= 10000
  CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
  CHECK(transfer.error() == "timeout");
}

TEST_CASE("Base64 decoding validates alphabet, padding and length") {
  SUBCASE("padding == 1 для остатка 2 байта") {
    std::string decoded;
    CHECK(SsvcLogProtocol::detail::decodeBase64("QUI=", decoded));
    CHECK(decoded == "AB");
  }
  SUBCASE("padding == 2 для остатка 1 байта") {
    std::string decoded = "stale";
    CHECK(SsvcLogProtocol::detail::decodeBase64("QQ==", decoded));
    CHECK(decoded == "A");
  }
  SUBCASE("padding не в хвосте группы отвергается") {
    std::string decoded;
    CHECK_FALSE(SsvcLogProtocol::detail::decodeBase64("AA=A", decoded));
    CHECK_FALSE(SsvcLogProtocol::detail::decodeBase64("A===", decoded));
  }
  SUBCASE("символ вне алфавита отвергается") {
    std::string decoded;
    CHECK_FALSE(SsvcLogProtocol::detail::decodeBase64("!!!!", decoded));
    CHECK_FALSE(SsvcLogProtocol::detail::decodeBase64("AA-A", decoded));
  }
  SUBCASE("длина не кратна 4 отвергается") {
    std::string decoded;
    CHECK_FALSE(SsvcLogProtocol::detail::decodeBase64("AAA", decoded));
  }
}

TEST_CASE("GET_LOG accepts chunk of exactly FILE_CHUNK_RAW_MAX bytes") {
  // 200 групп "AAAA" → 600 нулевых байт: ровно FILE_CHUNK_RAW_MAX.
  const std::string payload =
      std::string(R"({"type":"file","chunk":1,"data":")") + std::string(800, 'A') + "\"}";

  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);
  CHECK(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":1})"));
  CHECK(transfer.consume(payload.c_str()));
  CHECK(transfer.status() == SsvcLogProtocol::Status::COMPLETED);
  CHECK(transfer.data().size() == 600u);
}

TEST_CASE("Переход в ERROR освобождает собранный буфер") {
  SsvcLogProtocol::Transfer transfer;
  REQUIRE(transfer.beginFile(15));
  REQUIRE(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":2})"));
  REQUIRE(transfer.consume(R"({"type":"file","chunk":1,"data":"QUJD"})"));
  REQUIRE(transfer.data() == "ABC");

  // Нарушение нумерации переводит сессию в ошибку: удерживать десятки КБ
  // недоступного результата до следующего резерва нельзя.
  CHECK_FALSE(transfer.consume(R"({"type":"file","chunk":5,"data":"QUJD"})"));
  CHECK(transfer.status() == SsvcLogProtocol::Status::ERROR);
  CHECK(transfer.error() == "invalid_chunk");
  CHECK(transfer.data().empty());
}

TEST_CASE("base64 сверх лимита чанка отвергается до выделения памяти") {
  SsvcLogProtocol::Transfer transfer;
  REQUIRE(transfer.beginFile(15));
  REQUIRE(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":1})"));

  // 804 символа base64 — верхняя оценка 603 байта, больше FILE_CHUNK_RAW_MAX.
  const std::string frame =
      std::string(R"({"type":"file","chunk":1,"data":")") + std::string(804, 'A') + "\"}";
  CHECK_FALSE(transfer.consume(frame.c_str()));
  CHECK(transfer.error() == "invalid_data");
}

TEST_CASE("Список файлов сверх предела отвергается") {
  SsvcLogProtocol::Transfer transfer;
  REQUIRE(transfer.beginList());

  std::string files;
  for (std::size_t index = 0; index <= SsvcLogProtocol::LIST_FILES_MAX; ++index) {
    if (index != 0) {
      files += ',';
    }
    files += "\"" + std::to_string(index) + ".CSV\"";
  }
  const std::string frame =
      std::string(R"({"type":"response","request":"GET_LOG","result":"LIST","files":[)") +
      files + "]}";

  CHECK_FALSE(transfer.consume(frame.c_str()));
  CHECK(transfer.error() == "invalid_list");
}

TEST_CASE("requestVerb отделяет глагол от эха параметров") {
  // Контроллер повторяет в request всю команду: сравнение с "GET_LOG" целиком
  // не сработало бы на "GET_LOG 15", и кадр не продлил бы сессию передачи.
  CHECK(SsvcLogProtocol::detail::requestVerb("GET_LOG") == "GET_LOG");
  CHECK(SsvcLogProtocol::detail::requestVerb("GET_LOG 15") == "GET_LOG");
  CHECK(SsvcLogProtocol::detail::requestVerb("SET heads=[1,2]") == "SET");
}
