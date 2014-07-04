require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'

describe Rbuv::Prepare do
  include_context Rbuv::Loop
  it_should_behave_like Rbuv::Handle

  context "within a Rbuv::Loop#run_once" do
    it "is only called once" do
      on_prepare = double
      expect(on_prepare).to receive(:call).once.with(subject, nil)
      loop.run_once do
        subject.start do |*args|
          subject.stop
          on_prepare.call(*args)
        end
      end
    end
  end
end
