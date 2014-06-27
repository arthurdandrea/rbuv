shared_context Rbuv::Loop, :type => :handle do
  let(:loop) { Rbuv::Loop.new }
  after { loop.dispose }
  subject { described_class.new(loop) }
end
