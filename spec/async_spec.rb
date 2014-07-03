require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'

describe Rbuv::Async do
  include_context Rbuv::Loop
  it_should_behave_like Rbuv::Handle do
    subject do
      Rbuv::Async.new(loop) { }
    end
  end

  context "#send" do
    context "on another thread" do
      it "wakes the event loop" do
        on_async = double
        handle = Rbuv::Async.new(loop) do |*args|
          on_async.call(*args)
          handle.close
        end
        expect(on_async).to receive(:call).once.with(handle, nil)

        thread = Thread.start do
          handle.send
        end
        loop.run
        thread.join
      end
    end

    context "on the same thread" do
      it "wakes the event loop" do
        on_async = double
        handle = Rbuv::Async.new(loop) do |*args|
          on_async.call(*args)
          handle.close
        end
        expect(on_async).to receive(:call).once.with(handle, nil)

        loop.run do
          handle.send
        end
      end
    end
  end
end
