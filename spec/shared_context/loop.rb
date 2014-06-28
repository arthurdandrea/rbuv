shared_context Rbuv::Loop do
  let(:loop) { Rbuv::Loop.new }
  after { loop.dispose }
  subject { described_class.new(loop) }
end

shared_context "running Rbuv::Loop", loop: :running do
  around do |example|
    loop.run(&example)
  end
end
