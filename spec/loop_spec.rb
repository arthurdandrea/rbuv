require 'spec_helper'

describe Rbuv::Loop do
  context ".default" do
    it "returns a loop" do
      expect(Rbuv::Loop.default).to be_a Rbuv::Loop
    end
  end

  context "#handles" do
    after do subject.dispose end

    it "returns the associated handles" do
      timer = Rbuv::Timer.new subject
      expect(subject.handles).to eq([timer])
    end

    it "returns an empty array associated handles" do
      expect(subject.handles).to eq([])
    end
  end

  context "#dispose" do
    context "with no handles" do
      it "returns self" do
        expect(subject.dispose).to be(subject)
      end
    end

    context "with one handle" do
      let! (:timer) { Rbuv::Timer.new subject }

      it "cleans the handles array" do
        subject.dispose
        expect(subject.handles).to eq([])
      end

      it "close the handles" do
        subject.dispose
        expect(timer).to be_closed
      end
    end

    context "with two handles" do
      let! (:timer1) { Rbuv::Timer.new subject }
      let! (:timer2) { Rbuv::Timer.new subject }

      before do
        timer1.start 0, 0 do timer2.close end
      end

      it "cleans the handles array" do
        subject.dispose
        expect(subject.handles).to eq([])
      end

      it "close the handles" do
        subject.dispose
        expect(timer1).to be_closed
        expect(timer2).to be_closed
      end
    end
  end

  context "#run" do
    it "returns self" do
      expect(subject.run).to be(subject)
    end

    it "runs the passed block" do
      on_run = double
      expect(on_run).to receive(:call).once
      subject.run do
        on_run.call
      end
    end

    it "does not leave any open handles" do
      subject.run do
        open_handles = subject.handles.delete_if { |handle| handle.closing? }
        expect(open_handles).to eq([])
      end
    end

    it "passes the own loop to the block" do
      on_run = double
      expect(on_run).to receive(:call).once.with(subject)
      subject.run do |*args|
        on_run.call(*args)
      end
    end
  end
end
