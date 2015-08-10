require 'concurrent'
require 'concurrent/atomic/event'
require 'concurrent/atomic/atomic_boolean'

class MyTCPServer
  attr_reader :buffer

  def initialize(ip, port)
    @server = TCPServer.new ip, port
    @ready_event = Concurrent::Event.new
    @stoped_event = Concurrent::Event.new
    @stop_flag = Concurrent::AtomicBoolean.new false
    @buffer = ""
  end

  def _accept_loop
    @server.listen 1
    @ready_event.set
    client = @server.accept
    begin
      return if IO.select([], [], [client], 0)
      while true
        begin
          while (data = client.recv_nonblock(10)) != ""
            @buffer << data
          end
          break
        rescue Errno::EAGAIN
          break if @stop_flag.true?
          # do nothing
        end
        result = IO.select([client], [client], [client], 1)
        reads, writes, errors = result if result
        break unless errors.empty?
      end
    ensure
      client.close
    end
  rescue => exception
    puts exception.class
    puts exception
    puts exception.backtrace
  ensure
    begin
      @server.close unless @server.closed?
    rescue => exception
      puts exception.class
      puts exception
      puts exception.backtrace
    ensure
      @stoped_event.set
    end
  end

  def start
    @thread = Thread.new(&method(:_accept_loop))
    @ready_event.wait
  end

  def stop
    @stop_flag.make_true
    @stoped_event.wait
  end
end

shared_context "a tcp server" do
  def stop_server
    return nil if @tcp_server.nil?

    @tcp_server.stop
    buffer = @tcp_server.buffer
    @tcp_server = nil
    buffer
  end

  def open_server(ip, port)
    @tcp_server = MyTCPServer.new ip, port
    @tcp_server.start
  end
end

shared_context "an open tcp server" do |ip, port|
  include_context "a tcp server"

  before { open_server(ip, port) }
  after { stop_server }
end
