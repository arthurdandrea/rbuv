shared_examples Rbuv::Stream do
  it { is_expected.to be_a_kind_of Rbuv::Stream }

  describe "#write" do
    it_requires_a_block "data"

    it "calls the block" do
      on_write = double
      expect(on_write).to receive(:call).once
      subject.write "string" do |error|
        on_write.call(error)
      end
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
