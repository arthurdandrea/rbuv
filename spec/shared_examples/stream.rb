shared_examples Rbuv::Stream do
  describe "#write" do
    it "requires a block" do
      expect {
        stream.write "string"
      }.to raise_error LocalJumpError, 'no block given'
    end

    it "calls the block" do
      on_write = double
      expect(on_write).to receive(:call).once
      stream.write "string" do |error|
        on_write.call(error)
      end
    end

    it "requires a argument" do
      expect {
        stream.write { }
      }.to raise_error ArgumentError
    end

    it "requires a string" do
      expect {
        stream.write [] { }
      }.to raise_error TypeError
    end
  end
end
