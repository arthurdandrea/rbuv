require 'spec_helper'
require 'shared_examples/handle'
require 'shared_context/loop'

describe Rbuv::Check do
  include_context Rbuv::Loop
  it_should_behave_like Rbuv::Handle

  context "within a Rbuv::Loop#run_once" do
    it "is only called once" do
      on_check = double
      expect(on_check).to receive(:call).once.with(subject, nil)
      loop.run_once do
        subject.start do |*args|
          on_check.call(*args)
        end
      end
    end
  end
end
