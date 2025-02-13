//
//  simplesvr.cc
//
//  Copyright (c) 2019 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <cstdio>
#include <httplib.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>

#define SERVER_CERT_FILE "./cert.pem"
#define SERVER_PRIVATE_KEY_FILE "./key.pem"

using namespace httplib;
using namespace std;

string dump_headers(const Headers &headers) {
  string s;
  char buf[BUFSIZ];

  for (const auto &x : headers) {
    snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
    s += buf;
  }

  return s;
}

string dump_multipart_files(const MultipartFiles &files) {
  string s;
  char buf[BUFSIZ];

  s += "--------------------------------\n";

  for (const auto &x : files) {
    const auto &name = x.first;
    const auto &file = x.second;

    snprintf(buf, sizeof(buf), "name: %s\n", name.c_str());
    s += buf;

    snprintf(buf, sizeof(buf), "filename: %s\n", file.filename.c_str());
    s += buf;

    snprintf(buf, sizeof(buf), "content type: %s\n", file.content_type.c_str());
    s += buf;

    snprintf(buf, sizeof(buf), "text offset: %lu\n", file.offset);
    s += buf;

    snprintf(buf, sizeof(buf), "text length: %lu\n", file.length);
    s += buf;

    s += "----------------\n";
  }

  return s;
}

string log(const Request &req, const Response &res) {
  string s;
  char buf[BUFSIZ];

  s += "================================\n";

  snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(),
           req.version.c_str(), req.path.c_str());
  s += buf;

  string query;
  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto &x = *it;
    snprintf(buf, sizeof(buf), "%c%s=%s",
             (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
             x.second.c_str());
    query += buf;
  }
  snprintf(buf, sizeof(buf), "%s\n", query.c_str());
  s += buf;

  s += dump_headers(req.headers);
  s += dump_multipart_files(req.files);

  s += "--------------------------------\n";

  snprintf(buf, sizeof(buf), "%d\n", res.status);
  s += buf;
  s += dump_headers(res.headers);

  return s;
}

int main(int argc, const char **argv) {
  if (argc > 1 && string("--help") == argv[1]) {
    cout << "usage: simplesvr [PORT] [DIR]" << endl;
    return 1;
  }

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  SSLServer svr(SERVER_CERT_FILE, SERVER_PRIVATE_KEY_FILE);
#else
  Server svr;
#endif

  svr.Post("/multipart", [](const Request &req, Response &res) {
    auto body = dump_headers(req.headers) + dump_multipart_files(req.files);

    res.set_content(body, "text/plain");
  });

  svr.Post("/multi2", [](Stream& strm, const Request &req, Response &res) {
    std::ostringstream rsp;
    rsp << "req body stream:\n";
    MD5_CTX ctx;
    MD5_Init(&ctx);
    int nCall=0;

    detail::read_content(
        strm, req, std::numeric_limits<size_t>::max(), res.status
    ,   req.progress, [&] (const char *buf, size_t n){
            MD5_Update(&ctx, buf, n);
            nCall++;
            return true;
        }
    );
    unsigned char result[MD5_DIGEST_LENGTH+1];

    rsp << "nCall:" << nCall <<std::endl;

    MD5_Final(result, &ctx);
    rsp << "md5:" ;
    for(int i =0; i<MD5_DIGEST_LENGTH; i++) {
        rsp << std::hex << std::setw(2) << std::setfill('0') << (int) result[i];
    }
    rsp << std::endl;

    res.set_content(rsp.str(), "text/plain");
  });

  svr.set_error_handler([](const Request & /*req*/, Response &res) {
    const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
    char buf[BUFSIZ];
    snprintf(buf, sizeof(buf), fmt, res.status);
    res.set_content(buf, "text/html");
  });

  svr.set_logger(
      [](const Request &req, const Response &res) { cout << log(req, res); });

  auto port = 8080;
  if (argc > 1) { port = atoi(argv[1]); }

  auto base_dir = "./";
  if (argc > 2) { base_dir = argv[2]; }

  svr.set_base_dir(base_dir);

  cout << "The server started at port " << port << "...";

  svr.listen("localhost", port);

  return 0;
}
