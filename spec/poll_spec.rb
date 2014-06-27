require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'
require 'socket'

describe Rbuv::Poll do
  include_context Rbuv::Loop
  subject { Rbuv::Poll.new(loop, 0) }

  it_should_behave_like Rbuv::Handle

  def open_server_on(ip='127.0.0.1', port=60000)
    results = []
    rb_server = TCPServer.new ip, port
    rb_server.listen 10
    thread = Thread.start do
      while true
        client = rb_server.accept
        results << client.read
        client.close
      end
    end
    begin
      yield
      sleep 0.01 # give the network loop some time to breath
    ensure
      running = false
      thread.kill
      thread.join
      rb_server.close
    end
    results
  end

  context "when a socket is writable" do
    let(:socket) { TCPSocket.new('127.0.0.1', 60000) }
    subject { Rbuv::Poll.new(loop, socket.fileno) }

    around do |example|
      open_server_on('127.0.0.1', 60000) do
        example.run
      end
    end

    it "calls the block" do
      on_ready = double
      expect(on_ready).to receive(:call).once.with(subject, Rbuv::Poll::WRITABLE)

      loop.run do
        subject.start Rbuv::Poll::WRITABLE do |*args|
          on_ready.call(*args)
          subject.stop
        end
      end
    end
  end
end
