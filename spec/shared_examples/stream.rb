shared_examples Rbuv::Stream do
  it { is_expected.to be_a_kind_of Rbuv::Stream }

  describe "#write" do
    it_requires_a_block "data"

    it "calls the block" do
      on_write = double
      expect(on_write).to receive(:call).once
      subject.write "string" do |error|
        raise error if error
        on_write.call(error)
      end
      loop.run
    end

    it "returs a request" do
      write_req = subject.write "string" do |error|
        raise error if error
      end
      expect(write_req).to be_a_kind_of(Rbuv::Stream::WriteRequest)
    end

    it "can't be canceled" do
      write_req = subject.write "string" do |error|
        raise error if error
      end
      expect { write_req.cancel }.to raise_error Rbuv::Error, /invalid argument/i
    end

    it "one one one" do
      write_req = subject.write "string" do |error|
        raise error if error
      end
      expect(write_req.handle).to be(subject)
    end

    it "requires a argument" do
      expect {
        subject.write { }
      }.to raise_error ArgumentError
    end

    it "requires a string" do
      expect {
        subject.write [] { }
      }.to raise_error TypeError
    end
  end

  describe "#shutdown" do
    it_requires_a_block

    it "calls the block" do
      on_shutdown = double
      expect(on_shutdown).to receive(:call).once.with(nil)
      subject.shutdown do |error|
        on_shutdown.call(error)
      end
      loop.run
    end

    it "returns a request" do
      on_shutdown = double
      expect(on_shutdown).to receive(:call).once.with(nil)
      request = subject.shutdown do |error|
        on_shutdown.call(error)
      end
      expect(request).to be_a_kind_of(Rbuv::Stream::ShutdownRequest)
      loop.run
    end

    it "cannot be canceled" do
      on_shutdown = double
      expect(on_shutdown).to receive(:call).once.with(nil)
      request = subject.shutdown do |error|
        on_shutdown.call(error)
      end
      expect { request.cancel }.to raise_error Rbuv::Error, /invalid argument/i
      loop.run
    end

    it "forbids future writes" do
      on_shutdown = double
      expect(on_shutdown).to receive(:call).once.with(nil)
      on_write = double
      expect(on_write).to receive(:call).once.with(Rbuv::Error.new("broken pipe"))
      request = subject.shutdown do |error|
        on_shutdown.call(error)
      end
      loop.run
      subject.write "data" do |error|
        on_write.call(error)
      end
    end
  end
end
