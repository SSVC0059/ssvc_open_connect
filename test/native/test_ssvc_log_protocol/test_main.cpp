#include <doctest.h>

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
