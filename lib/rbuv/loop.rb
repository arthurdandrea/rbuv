module Rbuv
  class Loop
    # creates a {Rbuv::Tcp} associate with this loop
    # @return [Rbuv::Tcp] a fresh {Rbuv::Tcp} instance
    def tcp
      Tcp.new(self)
    end

    # creates a {Rbuv::Timer} associate with this loop
    # @return [Rbuv::Timer] a fresh {Rbuv::Timer} instance
    def timer
      Timer.new(self)
    end

    # creates a {Rbuv::Signal} associate with this loop
    # @return [Rbuv::Signal] a fresh {Rbuv::Signal} instance
    def signal
      Signal.new(self)
    end

    # runs the block on the next loop iteration
    # @yield right before the next loop iteration.
    # @yieldparam loop [self] the loop itself
    # @return [self] self
    # @raise [ArgumentError] if no block is given
    def next_tick(&block)
      raise ArgumentError unless block
      @next_tick_handle ||= Idle.new(self)
      @next_tick_handle.start do
        next_tick_callbacks = @next_tick_callbacks
        @next_tick_callbacks = []
        next_tick_callbacks.each do |callback|
          callback.call(self)
        end
        @next_tick_handle.stop if @next_tick_callbacks.empty?
      end unless @next_tick_handle.active?
      @next_tick_callbacks ||= []
      @next_tick_callbacks << block
      self
    end

    # Runs the event loop.
    #
    # If +run_mode+ is +:default+ Runs the event loop until the reference count
    # drops to zero.
    #
    # If +run_mode+ is +:once+ poll for new events once. Note that this function
    # blocks if there are no pending events.
    #
    # If +run_mode+ is +:nowait+ poll for new events once but don't block if
    # there are no pending events.
    #
    # @param run_mode [:default,:once,:nowait] the run mode
    # @yield If the block is passed, calls it right before the first loop
    #   iteration.
    # @yieldparam loop [self] the loop itself
    # @return [self] itself
    def run(mode=:default, &block)
      next_tick(&block) if block
      _run(mode)
    end

    # Poll for new events once. Note that this function blocks if there are no
    # pending events.
    # @yield [loop] If the block is passed, calls it right before the first loop
    #     iteration.
    # @yieldparam loop the loop itself
    # @return [self] itself
    def run_once(&block)
      next_tick(&block) if block
      _run(:once)
    end

    # Poll for new events once but don't block if there are no
    # pending events.
    #
    # @yield [loop] If the block is passed, calls it right before the first loop
    #     iteration.
    # @yieldparam loop the loop itself
    # @return [self] itself
    def run_nowait(&block)
      next_tick(&block) if block
      _run(:nowait)
    end

    # Tries to close every handle associated with this loop. It may call {#run}.
    # @return [self] itself
    # @raise [RuntimeError] if after disposal there are still some associated
    #   handle
    def dispose
      return self if self.handles.empty?
      self.handles.each do |handle|
        handle.close unless handle.closing?
      end
      run_nowait
      raise RuntimeError unless self.handles.empty?
      self
    end

    # Runs the loop using {#run} then ensure that {#dispose} is called
    # @yield (see #run)
    # @yieldparam loop (see #run)
    # @return (see #run)
    def run_and_dispose(&block)
      run(&block)
    ensure
      dispose
    end
  end
end
