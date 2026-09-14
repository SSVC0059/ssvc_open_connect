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

TEST_CASE("GET_LOG rejects malformed transfer packets") {
  SsvcLogProtocol::Transfer transfer;
  transfer.beginFile(15);
  CHECK_FALSE(transfer.consume(
      R"({"type":"response","request":"GET_LOG","result":"START","file":"15.CSV","total":0})"));
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
