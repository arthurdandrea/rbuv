require 'spec_helper'

describe Rbuv::Timer do
  let(:loop) { Rbuv::Loop.new }
  after { loop.dispose }
  subject { Rbuv::Timer.new(loop) }

  it { is_expected.to be_a_kind_of Rbuv::Handle }

  context "when timeout == 0" do
    context "#start" do
      it "when repeat == 0" do
        timer = Rbuv::Timer.new(loop)

        block = double
        expect(block).to receive(:call).once

        loop.run do
          timer.start 0, 0 do
            block.call
          end
        end
      end

      it "when repeat != 0" do
        timer = Rbuv::Timer.new(loop)

        block = double
        count_limit = 10
        expect(block).to receive(:call).exactly(count_limit)

        count = 0

        loop.run do
          timer.start 0, 1 do |t|
            block.call
            count += 1
            if count >= count_limit
              t.stop
            end
          end
        end
      end
    end # context "#start"

    it "#stop" do
      timer = Rbuv::Timer.new(loop)

      block = double
      expect(block).to receive(:call).once

      loop.run do
        timer.start 0, 1 do |t|
          block.call
          t.stop
        end
      end
    end

    context "#active?" do
      it "should be false" do
        timer = Rbuv::Timer.new(loop)

        loop.run do
          timer.start 0, 0 do |t|
            expect(t.active?).to be false
          end
        end
      end

      it "should be true" do
        timer = Rbuv::Timer.new(loop)

        loop.run do
          timer.start 0, 1 do |t|
            expect(t.active?).to be true
            t.stop
          end
        end
      end
    end

    context "#repeat" do
      [0, 10, 100].each do |repeat|
        it "should eq #{repeat}" do
          timer = Rbuv::Timer.new(loop)

          loop.run do
            timer.start 0, repeat do |t|
              expect(t.repeat).to eq repeat
              t.stop
            end
          end
        end
      end
    end

    context "#repeat=" do
      [0, 10, 100].each do |repeat|
        it "should eq #{repeat}" do
          timer = Rbuv::Timer.new(loop)

          loop.run do
            timer.start 0, 0 do |t|
              t.repeat = repeat
            end
          end
          expect(timer.repeat).to eq repeat
        end
      end
    end

    context ".start" do
      context 'be valid' do
        it "when repeat == 0" do
          block = double
          expect(block).to receive(:call).once

          loop.run do
            Rbuv::Timer.start(loop, 0, 0) { block.call }
          end
        end

        it "when repeat != 0" do
          block = double
          expect(block).to receive(:call).once

          loop.run do
            Rbuv::Timer.start loop, 0, 1 do |t|
              block.call
              t.stop
            end
          end
        end
      end

      context "won't segfault" do
        it "when repeat == 0" do
          loop.run do
            Rbuv::Timer.start(loop, 0, 0) {}
          end
          GC.start
        end

        it "when repeat != 0" do
          count = 0
          count_limit = 10

          loop.run do
            Rbuv::Timer.start loop, 0, 1 do |timer|
              GC.start
              count += 1
              if count >= count_limit
                timer.stop
              end
            end
          end
          GC.start
        end
      end

    end # context ".start"

  end # context "timeout == 0"

  context "when unreferenced" do
    # cant use double here, they are leaking to other specs
    it "does not leave the loop running" do
      unrefd_timer = Rbuv::Timer.new(loop).unref
      @timer_fired = false
      loop.run do
        unrefd_timer.start 1, 0 do
          @timer_fired = true
        end
      end
      expect(@timer_fired).to be false

      # make sure that the timer is fired before next spec
      loop.run do
        unrefd_timer.ref
      end
      expect(@timer_fired).to be true
    end

    it "call the close block however" do
      unrefd_timer = Rbuv::Timer.new(loop).unref
      @close_fired = false
      @timer_fired = false
      loop.run do
        unrefd_timer.start 1, 0 do
          @timer_fired = true
        end
        unrefd_timer.close do
          @close_fired = true
        end
      end
      expect(@timer_fired).to be false
      expect(@close_fired).to be true
    end

    # here we can use double, they are not leaking to other specs
    it "does not leave the loop running if there is another handler" do
      on_unrefd_timer = double
      on_refd_timer = double
      on_unrefd_close = double
      on_refd_close = double
      expect(on_unrefd_timer).not_to receive(:call)
      expect(on_refd_timer).to receive(:call).once
      expect(on_unrefd_close).to receive(:call).once
      expect(on_refd_close).to receive(:call).once
      unrefd_timer = Rbuv::Timer.new(loop).unref
      refd_timer = Rbuv::Timer.new(loop)
      loop.run do
        unrefd_timer.start 5, 0 do
          on_unrefd_timer.call
        end
        refd_timer.start 2, 0 do
          on_refd_timer.call
          refd_timer.close do
            on_refd_close.call
          end
          unrefd_timer.close do
            on_unrefd_close.call
          end
        end
      end
    end
  end
end
