module Rbuv
  class Timer
    def self.start(*args, &block)
      if args.first.is_a? Rbuv::Loop
        timer = new(args.shift)
      else
        timer = new
      end
      timer.start(*args, &block)
    end
  end
end
