module Rbuv
  class Signal
    ::Signal.list.each do |signame, signum|
      const_set signame, signum
    end
    def self.start(*args, &block)
      if args.first.is_a? Rbuv::Loop
        signal = new(args.shift)
      else
        signal = new
      end
      signal.start(*args, &block)
    end
  end
end
