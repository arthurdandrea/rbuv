module Rbuv
  class Signal
    ::Signal.list.each do |signame, signum|
      const_set signame, signum
    end

    # Creates a new signal and calls {#start} on it, starting to watch for
    # signal +signum+
    # @overload start(signum)
    #   Runs the new signal handle in the {Rbuv::Loop.default}
    #   @param signum [Number] the signal number to watch for
    #   @yield Calls the block when the +signum+ is recieved
    #   @yieldparam signal [self] itself
    #   @yieldparam signum [Number] the signal number recieved
    # @overload start(loop, signum)
    #   Runs the new signal handle in the given loop
    #   @param loop [Rbuv::Loop] the loop where this handle runs
    #   @param signum [Number] the signal number to watch for
    #   @yield Calls the block when the +signum+ is recieved
    #   @yieldparam signal [self] itself
    #   @yieldparam signum [Number] the signal number recieved
    # @return [Rbuv::Signal] a fresh started {Rbuv::Signal} watching for
    #   +signum+
    # @see #start
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
