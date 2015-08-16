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
end
