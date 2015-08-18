require 'spec_helper'
require 'shared_context/loop'
require 'pp'
require 'fiber'

describe Rbuv::GetaddrinfoRequest do
  include_context Rbuv::Loop
  let(:expected_result) {
    expected_result = Socket.getaddrinfo("google.com", "ftp")
    expected_result.each do |arr|
      arr.delete_at(3)
    end
    expected_result
  }

  it "works" do
    block = double("block")
    expect(block).to receive(:call).once.with(expected_result, nil)
    subject = Rbuv::GetaddrinfoRequest.new("google.com", "ftp", loop) do |result, error|
      block.call(result, error)
    end
    loop.run
  end
end
