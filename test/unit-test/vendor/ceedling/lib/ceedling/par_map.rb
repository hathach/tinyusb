

def par_map(n, things, &block)
  queue = Queue.new
  things.each { |thing| queue << thing }
  threads = (1..n).collect do
    Thread.new do
      begin
        while true
          yield queue.pop(true)
        end
      rescue ThreadError

      end
    end
  end
  threads.each { |t| t.join }
end
