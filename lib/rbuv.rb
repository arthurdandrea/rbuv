require 'rbuv/rbuv'
require 'rbuv/version'
require 'rbuv/timer'
require 'rbuv/signal'
require 'rbuv/loop'

module Rbuv
  class << self

    # Stop the default loop.
    # @see Rbuv::Loop#stop
    # @return [Rbuv::Loop] the {Rbuv::Loop.default}
    def stop_loop
      Loop.default.stop
    end

    alias stop stop_loop

    # Run the default loop.
    # @see Rbuv::Loop#run
    # @yield (see Rbuv::Loop#run)
    # @yieldparam loop [Rbuv::Loop] the {Rbuv::Loop.default}
    # @return [Rbuv::Loop] the {Rbuv::Loop.default}
    def run_loop(&block)
      Loop.default.run(&block)
    end

    alias run run_loop

    # Run the default loop once.
    # @see Rbuv::Loop#run_once
    # @yield (see Rbuv::Loop#run_once)
    # @yieldparam loop [Rbuv::Loop] the {Rbuv::Loop.default}
    # @return [Rbuv::Loop] the {Rbuv::Loop.default}
    def run_block(&block)
      Loop.default.run_once(&block)
    end

  end
end
