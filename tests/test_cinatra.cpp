#include <filesystem>
#include <future>
#include <system_error>

#include "cinatra.hpp"
#include "cinatra/client_factory.hpp"
#include "cinatra/http_client.hpp"
#include "doctest.h"

using namespace cinatra;
void print(const response_data &result) {
  print(result.ec, result.status, result.resp_body, result.resp_headers.second);
}

TEST_CASE("test multiple ranges download") {
  coro_http_client client{};
  std::string uri = "http://uniquegoodshiningmelody.neverssl.com/favicon.ico";

  std::string filename = "test1.txt";
  std::error_code ec{};
  std::filesystem::remove(filename, ec);
  resp_data result = async_simple::coro::syncAwait(
      client.async_download(uri, filename, "1-10,11-16"));
  if (result.status == status_type::ok) {
    CHECK(std::filesystem::file_size(filename) == 16);
  }
}

TEST_CASE("test ranges download") {
  coro_http_client client{};
  std::string uri = "http://httpbin.org/range/32";

  std::string filename = "test1.txt";
  std::error_code ec{};
  std::filesystem::remove(filename, ec);
  resp_data result = async_simple::coro::syncAwait(
      client.async_download(uri, filename, "1-10"));
  if (result.status == status_type::ok) {
    CHECK(std::filesystem::file_size(filename) == 10);
  }

  filename = "test2.txt";
  std::filesystem::remove(filename, ec);
  result = async_simple::coro::syncAwait(
      client.async_download(uri, filename, "10-15"));
  if (result.status == status_type::ok) {
    CHECK(std::filesystem::file_size(filename) == 6);
  }
  // multiple range test
  //  auto result =
  //      async_simple::coro::syncAwait(client.async_download(uri, "test2.txt",
  //      "1-10, 20-30"));
  //  if (result.resp_body.size() == 31)
  //    CHECK(result.resp_body == "bcdefghijklmnopqrstuvwxyzabcdef");
}

TEST_CASE("test coro_http_client quit") {
  std::promise<bool> promise;
  [&] {
    { coro_http_client client{}; }
    promise.set_value(true);
  }();

  CHECK(promise.get_future().get());
}

TEST_CASE("test coro_http_client chunked download") {
  coro_http_client client{};
  std::string uri =
      "http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx";
  std::string filename = "test.jpg";

  std::error_code ec{};
  std::filesystem::remove(filename, ec);
  auto r = client.download(uri, filename);
  CHECK(!r.net_err);
  CHECK(r.status == status_type::ok);

  filename = "test2.jpg";
  std::filesystem::remove(filename, ec);
  r = client.download(uri, filename);
  CHECK(!r.net_err);
  CHECK(r.status == status_type::ok);

  SUBCASE("test the correctness of the downloaded file") {
    auto self_http_client = client_factory::instance().new_client();
    std::string self_filename = "_" + filename;

    std::promise<bool> pro;
    auto fu = pro.get_future();

    std::error_code ec;
    std::filesystem::remove(self_filename, ec);
    self_http_client->download(uri, self_filename, [&](response_data data) {
      if (data.ec) {
        std::cout << data.ec.message() << "\n";
        pro.set_value(false);
        return;
      }

      std::cout << "finished download\n";
      pro.set_value(true);
    });

    REQUIRE(fu.get());

    auto read_file = [](const std::string &filename) {
      std::string file_content;
      std::ifstream ifs(filename, std::ios::binary);
      std::array<char, 1024> buff;
      while (ifs.read(buff.data(), buff.size())) {
        file_content.append(std::string_view{
            buff.data(),
            static_cast<std::string_view::size_type>(ifs.gcount())});
      }
      return file_content;
    };
    auto f1 = read_file(filename);
    auto f2 = read_file(self_filename);

    REQUIRE(f1.size() == f2.size());
    CHECK(f1 == f2);
  }
}

TEST_CASE("test coro_http_client get") {
  coro_http_client client{};
  auto r = client.get("http://www.purecpp.cn");
  CHECK(!r.net_err);
  CHECK(r.status == status_type::ok);
}

TEST_CASE("test coro_http_client add header and url queries") {
  coro_http_client client{};
  client.add_header("Connection", "keep-alive");
  auto r =
      async_simple::coro::syncAwait(client.async_get("http://www.purecpp.cn"));
  CHECK(!r.net_err);
  CHECK(r.status == status_type::ok);

  auto r2 = async_simple::coro::syncAwait(
      client.async_get("http://www.baidu.com?name='tom'&age=20"));
  CHECK(!r2.net_err);
  CHECK(r2.status == status_type::ok);
}

TEST_CASE("test coro_http_client not exist domain and bad uri") {
  {
    coro_http_client client{};
    auto r = async_simple::coro::syncAwait(
        client.async_get("http://www.notexistwebsit.com"));
    CHECK(r.net_err);
    CHECK(r.status != status_type::ok);
    CHECK(client.has_closed());
  }

  {
    coro_http_client client{};
    auto r = async_simple::coro::syncAwait(client.async_get("www.purecpp.cn"));
    CHECK(r.net_err);
    CHECK(r.status != status_type::ok);
    CHECK(client.has_closed());
  }
}

TEST_CASE("test coro_http_client async_get") {
  coro_http_client client{};
  auto r =
      async_simple::coro::syncAwait(client.async_get("http://www.purecpp.cn"));
  CHECK(!r.net_err);
  CHECK(r.status == status_type::ok);

  auto r1 =
      async_simple::coro::syncAwait(client.async_get("http://www.baidu.com"));
  CHECK(!r.net_err);
  CHECK(r.status == status_type::ok);
}

TEST_CASE("test coro_http_client async_connect") {
  coro_http_client client{};
  auto r =
      async_simple::coro::syncAwait(client.async_ping("http://www.purecpp.cn"));
  CHECK(r);
}

TEST_CASE("test basic http request") {
  http_server server(std::thread::hardware_concurrency());
  bool r = server.listen("0.0.0.0", "8090");
  if (!r) {
    std::cout << "listen failed."
              << "\n";
  }

  server.set_http_handler<GET>(
      "/", [&server](request &, response &res) mutable {
        res.set_status_and_content(status_type::ok, "hello world");
      });
  server.set_http_handler<POST>(
      "/", [&server](request &req, response &res) mutable {
        std::string str(req.body());
        str.append(" reply from post");
        res.set_status_and_content(status_type::ok, std::move(str));
      });

  std::promise<void> pr;
  std::future<void> f = pr.get_future();
  std::thread server_thread([&server, &pr]() {
    pr.set_value();
    server.run();
  });
  f.wait();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  coro_http_client client{};
  std::string uri = "http://127.0.0.1:8090";
  resp_data result = async_simple::coro::syncAwait(client.async_get(uri));
  CHECK(result.resp_body == "hello world");

  result = async_simple::coro::syncAwait(client.async_post(
      uri, "hello coro_http_client", req_content_type::string));
  CHECK(result.resp_body == "hello coro_http_client reply from post");

  server.stop();
  server_thread.join();
}
