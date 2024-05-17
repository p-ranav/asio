#include <iostream>
#include <ostream>
#include <istream>
#include <ctime>
#include <string>
#include <asio.hpp>
#include <functional>
#include <thread>

using asio::ip::tcp;
using namespace std::placeholders;

class HttpServer; // forward declaration

class Request : public std::enable_shared_from_this<Request> {
public:
  static std::string make_daytime_string() {
    std::time_t now = std::time(0);
    return std::ctime(&now);
  }

  Request(HttpServer& server, asio::io_service& io_service)
      : server(server), io_service(io_service), socket(io_service) {}

  void start() {
    // Read request until the end
    asio::async_read_until(socket, request, "\r\n\r\n",
                           std::bind(&Request::afterRead, shared_from_this(), _1, _2));
  }

  tcp::socket& get_socket() {
    return socket;
  }

private:
  void afterRead(const std::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
      return;
    }

    // Prepare response
    asio::streambuf response;
    std::ostream res_stream(&response);

    std::string time = make_daytime_string();

    res_stream << "HTTP/1.0 200 OK\r\n"
            << "Content-Type: text/html; charset=UTF-8\r\n"
            << "Content-Length: " << time.length() + 206 << "\r\n\r\n"
            << "<html><head><title>Hello</title>"
            << "<script>"
            << "function prettyPrintTime(hours, minutes, seconds) {"
            << "return (hours.toString().padStart(2, '0') + ':' + minutes.toString().padStart(2, '0') + ':' + seconds.toString().padStart(2, '0'));"
            << "}"
            << "function prettyPrintDate(date) {"
            << "var days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];"
            << "var months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];"
            << "var dayOfWeek = days[date.getDay()];"
            << "var month = months[date.getMonth()];"
            << "var day = date.getDate();"
            << "var year = date.getFullYear();"
            << "return dayOfWeek + ', ' + month + ' ' + day + ', ' + year;"
            << "}"
            << "function updateTime() {"
            << "var currentTime = new Date();"
            << "var hours = currentTime.getHours();"
            << "var minutes = currentTime.getMinutes();"
            << "var seconds = currentTime.getSeconds();"
            << "document.getElementById('time').innerHTML = prettyPrintTime(hours, minutes, seconds);"
            << "document.getElementById('date').innerHTML = prettyPrintDate(currentTime);"
            << "}"
            << "setInterval(updateTime, 1000);"
            << "</script>"
            << "</head><body>"
            << "<div id='date'></div>"
            << "<div id='time'></div></body></html>";

    // Write response asynchronously
    asio::async_write(socket, response,
                      std::bind(&Request::afterWrite, shared_from_this(), _1, _2));
  }

  void afterWrite(const std::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
      return;
    }

    // Close connection after writing
    socket.close();
  }

  HttpServer& server;
  asio::io_service& io_service;
  asio::streambuf request;
  tcp::socket socket;
};

#include <chrono>

class HttpServer {
public:
  HttpServer(unsigned int port, asio::io_service& io_service)
      : acceptor(io_service, tcp::endpoint(tcp::v4(), port)), io_service(io_service) {}

  void initialize() {
    start_accept();
  }

  void step() {
    // Measure execution time using std::chrono
    auto start = std::chrono::high_resolution_clock::now();

    // Process any pending events from io_service
    io_service.poll();
    // Schedule another step for next iteration (assuming 80ms RTOS call)
    io_service.post([this] { });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Print execution time in microseconds
    std::cout << "HttpServer::step() execution time: " << duration.count() << " microseconds" << std::endl;
  }

private:
  void start_accept() {
    std::shared_ptr<Request> req(new Request(*this, io_service));
    acceptor.async_accept(req->get_socket(),
                          std::bind(&HttpServer::handle_accept, this, req, _1));
  }

  void handle_accept(std::shared_ptr<Request> req, const std::error_code& error) {
    if (!error) {
      req->start(); // Start handling the request
    }
    start_accept(); // Continuously accept new connections
  }

  tcp::acceptor acceptor;
  asio::io_service& io_service;
};

asio::io_service io_service;

int main() {
  HttpServer server(8080, io_service);
  server.initialize();

  auto start_time = std::chrono::high_resolution_clock::now();
  while (true) {
    server.step();

    // Calculate time taken by step()
    auto end_time = std::chrono::high_resolution_clock::now();
    auto step_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    start_time = end_time; // Reset start time for next iteration

    // Calculate remaining sleep time for 80ms rate
    auto target_duration = std::chrono::microseconds(80000);
    auto sleep_duration = target_duration - step_duration;

    // Ensure sleep duration is non-negative (avoid negative sleep)
    if (sleep_duration.count() > 0) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_until(start_time + sleep_duration);
    }

  }
}
