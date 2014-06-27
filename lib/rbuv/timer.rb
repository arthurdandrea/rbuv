module Rbuv
  class Timer
    def self.one_shot(loop=nil, &block)
      timer = new(loop)
      timer.start(0, 0) do
        timer.close
        block.call
      end
      nil
    end

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
