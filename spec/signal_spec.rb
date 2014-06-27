require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'

describe Rbuv::Signal do
  include_context Rbuv::Loop
  it_should_behave_like Rbuv::Handle

  it "#start" do
    block = double
    expect(block).to receive(:call).once

    loop.run do
      sig = Rbuv::Signal.new(loop)
      sig.start 2 do
        block.call
        sig.close
      end

      `kill -2 #{Process.pid}`
    end
  end

  it "#stop" do
    block = double
    expect(block).to receive(:call).once

    loop.run do
      sig = Rbuv::Signal.new(loop)
      sig.start 2 do
        block.call
        sig.stop
      end

      `kill -2 #{Process.pid}`
    end
  end
end
