module Rbuv
  class Loop
    def tcp
      Tcp.new(self)
    end

    def timer
      Timer.new(self)
    end

    def signal
      Signal.new(self)
    end


    def run_with_block(&block)
      Timer.new(self).start(0, 0, &block) if block
      run_without_block
    end
    alias_method :run_without_block, :run
    alias_method :run, :run_with_block
  end
end
