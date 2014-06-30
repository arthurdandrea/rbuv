require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'
require 'socket'
require 'shared_context/tcp_utils'

describe Rbuv::Poll do
  include_context Rbuv::Loop

  subject { Rbuv::Poll.new(loop, 0) }

  it_should_behave_like Rbuv::Handle

  context "when a socket is writable" do
    let(:socket) { TCPSocket.new('127.0.0.1', 60000) }
    subject { Rbuv::Poll.new(loop, socket.fileno) }
    include_context "an open tcp server", '127.0.0.1', 60000

    it "calls the block" do
      on_ready = double
      expect(on_ready).to receive(:call).once.with(subject, Rbuv::Poll::WRITABLE, nil)

      loop.run do
        subject.start Rbuv::Poll::WRITABLE do |*args|
          on_ready.call(*args)
          subject.stop
        end
      end
    end
  end
end
