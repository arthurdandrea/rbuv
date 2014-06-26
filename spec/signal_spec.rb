require 'spec_helper'

describe Rbuv::Signal do
  it "#start" do
    block = double
    block.should_receive(:call).once

    Rbuv.run do
      sig = Rbuv::Signal.new
      sig.start 2 do
        block.call
        sig.close
      end

      Process.kill(2, Process.pid)
    end
  end

  it "#stop" do
    block = double
    block.should_receive(:call).once

    Rbuv.run do
      sig = Rbuv::Signal.new
      sig.start 2 do
        block.call
        sig.stop
      end

      Process.kill(2, Process.pid)
    end
  end
end
