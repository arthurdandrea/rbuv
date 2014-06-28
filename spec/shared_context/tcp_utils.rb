shared_context "a tcp server" do
  def stop_server
    return @tcp_server_reads if @tcp_server.nil?
    sleep 0.01
    @tcp_server_thread.kill
    @tcp_server_thread.join
    @tcp_server.close
    @tcp_server = nil
    @tcp_server_reads
  end

  def open_server(ip, port)
    @tcp_server = TCPServer.new ip, port
    @tcp_server_reads = []
    @tcp_server.listen 10
    @tcp_server_thread = Thread.start do
      begin
        while true
          client = @tcp_server.accept
          @tcp_server_reads << client.read
          client.close
        end
      rescue => exception
        puts exception
        puts exception.backtrace
      end
    end
  end
end

shared_context "an open tcp server" do |ip, port|
  include_context "a tcp server"

  before { open_server(ip, port) }
  after { stop_server }
end
