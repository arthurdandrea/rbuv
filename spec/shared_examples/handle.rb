shared_examples Rbuv::Handle do
  context "#ref" do
    it "respond_to #ref" do
      expect(subject).to respond_to(:ref)
    end
  end

  context "#ref" do
    it "respond_to #ref" do
      expect(subject).to respond_to(:ref)
    end
  end

  context "#unref" do
    it "respond_to #unref" do
      expect(subject).to respond_to(:unref)
    end
  end

  context "#close" do
    it "respond_to #close" do
      expect(subject).to respond_to(:close)
    end
  end

  context "#active?" do
    it "respond_to #active?" do
      expect(subject).to respond_to(:active?)
    end
  end

  context "#close" do
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
