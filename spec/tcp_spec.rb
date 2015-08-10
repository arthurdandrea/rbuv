require 'spec_helper'
require 'shared_examples/stream'
require 'shared_examples/handle'
require 'shared_context/loop'
require 'shared_context/tcp_utils'
require 'socket'

def port_in_use?(port, host='127.0.0.1')
  s = TCPServer.new host, port
  s.close
  false
rescue Errno::EADDRINUSE
  true
end

def port_alive?(port, host='127.0.0.1')
  s = TCPSocket.new host, port
  s.close
  false
rescue Errno::ECONNREFUSED
  true
end

describe Rbuv::Tcp, :type => :handle do
  include_context Rbuv::Loop
  it_should_behave_like Rbuv::Handle
  it_should_behave_like Rbuv::Stream do
    include_context "an open tcp server", '127.0.0.1', 60000

    before do
      loop.run do
        subject.connect '127.0.0.1', 60000 do
        end
      end
    end
  end

  it "#bind" do
    if RUBY_PLATFORM.downcase.include?("linux")
      pending "this spec does't pass on linux machines, see #1 on github"
    end

    expect(port_in_use?(60000)).to be false

    loop.run do

      begin
        subject.bind '127.0.0.1', 60000

        expect(port_in_use?(60000)).to be true
      ensure
        subject.close
      end

      expect(port_in_use?(60000)).to be false
    end
  end

  context "#listen" do
    it "when address not in use" do
      expect(port_in_use?(60000)).to be false

      loop.run do
        begin
          subject.bind '127.0.0.1', 60000
          subject.listen(10) { Rbuv.stop_loop }

          expect(port_in_use?(60000)).to be true
        ensure
          subject.close
        end

        expect(port_in_use?(60000)).to be false
      end
    end

    it "when address already in use" do
      expect(port_in_use?(60000)).to be false

      loop.run do
        begin
          s = TCPServer.new '127.0.0.1', 60000

          subject.bind '127.0.0.1', 60000
          expect { subject.listen(10) {} }.to raise_error
        ensure
          s.close
          subject.close
        end
      end
    end

    it "should call the on_connection callback when connection coming" do
      on_connection = double
      expect(on_connection).to receive(:call).once.with(subject, nil)

      loop.run do

        subject.bind '127.0.0.1', 60000

        subject.listen(10) do |*args|
          on_connection.call(*args)
          subject.close
        end

        sock = TCPSocket.new '127.0.0.1', 60000
        sock.close
      end
    end
  end

  context "#accept" do
    before do
      expect(port_in_use?(60000)).to be false
      subject.bind '127.0.0.1', 60000
    end

    context "with a client as a paramenter" do
      it "does not raise an error" do
        loop.run do
          sock = nil

          subject.listen(10) do |s|
            client = Rbuv::Tcp.new(loop)
            expect { subject.accept(client) }.not_to raise_error
            sock.close
            subject.close
          end

          sock = TCPSocket.new '127.0.0.1', 60000
        end
      end
    end

    context "with no parameters" do
      it "returns a Rbuv::Tcp" do
        loop.run do
          sock = nil
          subject.listen(10) do |s|
            client = s.accept
            expect(client).to be_a Rbuv::Tcp
            sock.close
            subject.close
          end

          sock = TCPSocket.new '127.0.0.1', 60000
        end
      end

      it "does not return self" do
        loop.run do
          sock = nil

          subject.listen(10) do |s|
            client = s.accept
            expect(client).not_to be s
            sock.close
            subject.close
          end

          sock = TCPSocket.new '127.0.0.1', 60000
        end
      end
    end
  end

  context "#connect" do
    context "when server does not exist" do
      it "calls the block with tcp and an error" do
        on_connect = double
        expect(on_connect).to receive(:call).once.with(subject, Rbuv::Error.new('connection refused'))

        loop.run do
          subject.connect('127.0.0.1', 60000) do |*args|
            on_connect.call(*args)
            subject.close
          end
        end
      end
    end

    context "when server exists" do
      include_context "an open tcp server", '127.0.0.1', 60000

      it "calls the block with tcp and nil error" do
        on_connect = double
        expect(on_connect).to receive(:call).once.with(subject, nil)

        loop.run do
          subject.connect('127.0.0.1', 60000) do |*args|
            on_connect.call(*args)
            subject.close
          end
        end
      end
    end
  end

  context "#write" do
    include_context "an open tcp server", '127.0.0.1', 60000

    it "calls the block with " do
      on_write = double
      expect(on_write).to receive(:call).once.with(nil)

      loop.run do
        subject.connect('127.0.0.1', 60000) do
          subject.write('test string') do |*args|
            on_write.call(*args)
            subject.close
          end
        end
      end
    end

    it "writes to the stream" do
      loop.run do
        subject.connect('127.0.0.1', 60000) do
          subject.write('test string') do
            subject.close
          end
        end
      end
      results = stop_server
      expect(results).to eq('test string')
    end
  end
end
