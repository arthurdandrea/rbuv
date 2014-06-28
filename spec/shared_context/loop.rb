shared_context Rbuv::Loop do
  let(:loop) { Rbuv::Loop.new }
  after { loop.dispose }
  subject { described_class.new(loop) }
end
