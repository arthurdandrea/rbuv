require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'

describe Rbuv::Idle do
  include_context Rbuv::Loop
  it_should_behave_like Rbuv::Handle

  context "within a Rbuv::Loop#run_once" do
    it "is only called once" do
      on_idle = double("Rbuv::Idle callback")

      expect(on_idle).to receive(:call).once.with(subject, nil)
      subject.start do |*args|
        subject.stop
        on_idle.call(*args)
      end

      loop.run_once
    end

    it "is called before Rbuv::Prepare" do
      idle = subject
      prepare = Rbuv::Prepare.new(loop)
      on_idle = double("Rbuv::Idle callback")
      on_prepare = double("Rbuv::Prepare callback")

      expect(on_idle).to receive(:call).once.with(idle, nil).ordered
      expect(on_prepare).to receive(:call).once.with(prepare, nil).ordered

      prepare.start do |*args|
        prepare.stop
        on_prepare.call(*args)
      end
      idle.start do |*args|
        idle.stop
        on_idle.call(*args)
      end

      loop.run_once
    end
  end
end
