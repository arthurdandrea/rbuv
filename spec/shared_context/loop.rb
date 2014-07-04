shared_context Rbuv::Loop do
  let(:loop) { Rbuv::Loop.new }
  after { loop.dispose }
  subject { described_class.new(loop) }

  def trigger_io
    @io_trigger ||= begin
      io_trigger = Rbuv::Tcp.new(loop)
      io_trigger.connect("127.0.0.1", 6000) { }
      io_trigger
    end
  end
end
