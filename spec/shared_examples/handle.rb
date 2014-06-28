shared_examples Rbuv::Handle do
  it { is_expected.to be_a_kind_of Rbuv::Handle }

  context "#ref" do
    it { is_expected.to respond_to(:ref) }

    it "return self" do
      expect(subject.ref).to be subject
    end
  end

  context "#unref" do
    it { is_expected.to respond_to(:unref) }

    it "return self" do
      expect(subject.unref).to be subject
    end
  end

  context "#active?" do
    it { is_expected.to respond_to(:active?) }
  end

  context "#close" do
    it "returns self" do
      expect(subject.close).to be subject
    end

    it "affect #closing?" do
      loop.run do
        subject.close
        expect(subject.closing?).to be true
      end
    end

    it "affect #closed?" do
      loop.run do
        subject.close do
          expect(subject.closed?).to be true
        end
      end
    end

    it "call once" do
      on_close = double
      expect(on_close).to receive(:call).once

      loop.run do
        subject.close do
          on_close.call
        end
      end
    end

    it "call multi-times" do
      on_close = double
      expect(on_close).to receive(:call).once

      no_on_close = double
      expect(no_on_close).not_to receive(:call)

      loop.run do

        subject.close do
          on_close.call
        end

        subject.close do
          no_on_close.call
        end
      end
    end
  end
end
