require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'

describe Rbuv::Signal do
  include_context Rbuv::Loop
  it_should_behave_like Rbuv::Handle

  it "#start" do
    loop.run do
      block = double
      expect(block).to receive(:call).once.with(subject, 2)

      subject.start 2 do |*args|
        block.call(*args)
        subject.close
      end

      `kill -2 #{Process.pid}`
    end
  end

  it "#stop" do
    loop.run do
      block = double
      expect(block).to receive(:call).once.with(subject, 2)

      subject.start 2 do |*args|
        block.call(*args)
        subject.stop
      end

      `kill -2 #{Process.pid}`
    end
  end
end
