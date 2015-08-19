shared_examples Rbuv::Handle do
  it { is_expected.to be_a_kind_of Rbuv::Handle }

  context "#loop" do
    it { is_expected.to respond_to(:loop) }

    context "when the handle is closed" do
      before do
        subject.close
        loop.run_nowait until subject.closed?
      end

      it "return nil" do
        expect(subject.loop).to be nil
      end
    end

    context "when the handle is not closed" do
      it "return the loop" do
        expect(subject.loop).to be loop
      end
    end
  end

  context "#ref?" do
    it { is_expected.to respond_to(:ref?) }

    it "returns the ref state" do
      subject.ref
      expect(subject.ref?).to be true
      subject.unref
      expect(subject.ref?).to be false
    end
  end

  context "#ref" do
    it { is_expected.to respond_to(:ref) }

    it "return self" do
      expect(subject.ref).to be subject
    end

    it "change #ref?" do
      subject.unref
      expect{ subject.ref }.to change{ subject.ref? }.from(false).to(true)
    end

    it_raise_error_when_closed
  end

  context "#unref" do
    it { is_expected.to respond_to(:unref) }

    it "return self" do
      expect(subject.unref).to be subject
    end

    it "change #ref?" do
      subject.ref
      expect{ subject.unref }.to change{ subject.ref? }.from(true).to(false)
    end

    it_raise_error_when_closed
  end

  context "#active?" do
    it { is_expected.to respond_to(:active?) }

    it_raise_error_when_closed
  end

  context "#close" do
    it "returns self" do
      expect(subject.close).to be subject
    end

    it "affect #closing?" do
      loop.run do
        subject.close
        expect(subject).to be_closing
      end
    end

    it "affect #closed?" do
      loop.run do
        subject.close do
          expect(subject).to be_closed
        end
      end
    end

    it "call once" do
      on_close = double
      expect(on_close).to receive(:call).once.with(subject)

      loop.run do
        subject.close do |*args|
          on_close.call(*args)
        end
      end
    end

    it "call multi-times" do
      on_close = double
      expect(on_close).to receive(:call).once.with(subject)

      no_on_close = double
      expect(no_on_close).not_to receive(:call)

      loop.run do

        subject.close do |*args|
          on_close.call(*args)
        end

        subject.close do |*args|
          no_on_close.call(*args)
        end
      end
    end
  end
end
