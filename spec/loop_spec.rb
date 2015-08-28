require 'spec_helper'

describe Rbuv::Loop do
  after do
    subject.dispose
  end

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

  shared_examples "#run" do |&block|
    context "when no block is passed" do
      it "returns self" do
        expect(run).to be(subject)
      end

      it "does not leave any open handles" do
        run
        open_handles = subject.handles.delete_if { |handle| handle.closing? }
        expect(open_handles).to eq([])
      end
    end

    context "when a block is passed" do
      it "returns self" do
        expect(run { }).to be(subject)
      end

      it "does not leave any open handles " do
        on_run = spy("Rbuv::Loop#run callback")
        run do |*args|
          on_run.call(*args)
        end
        expect(on_run).to have_received(:call).once
        open_handles = subject.handles.delete_if { |handle| handle.closing? }
        expect(open_handles).to eq([])
      end

      it "runs the passed block" do
        on_run = spy("Rbuv::Loop#run callback")
        run do |*args|
          on_run.call(*args)
        end
        expect(on_run).to have_received(:call).once
      end

      it "passes the own loop to the block" do
        on_run = spy("Rbuv::Loop#run callback")
        run do |*args|
          on_run.call(*args)
        end
        expect(on_run).to have_received(:call).once.with(subject)
      end
    end
  end

  context "#run_once" do
    def run(&block)
      subject.run_once(&block)
    end
    include_examples "#run"
  end

  context "#run_nowait" do
    def run(&block)
      subject.run_nowait(&block)
    end
    include_examples "#run"
  end

  context "#run(:nowait)" do
    def run(&block)
      subject.run(:nowait, &block)
    end
    include_examples "#run"
  end

  context "#run(:once)" do
    def run(&block)
      subject.run(:once, &block)
    end
    include_examples "#run"
  end

  context "#run" do
    def run(&block)
      subject.run(&block)
    end
    include_examples "#run"
  end

  context "#now" do
    it "is cached" do
      cached_now = subject.now
      5.times do
        expect(subject.now).to eq(cached_now)
        sleep(0.001)
      end
    end

    it "changes on different runs" do
      start = subject.now
      while subject.now - start < 500
        subject.run_nowait
      end
    end
  end

  context "#update_time" do
    it "changes #now" do
      cached_now = subject.now
      sleep(0.001)
      subject.update_time
      expect(subject.now).to_not eq(cached_now)
    end
  end
end
