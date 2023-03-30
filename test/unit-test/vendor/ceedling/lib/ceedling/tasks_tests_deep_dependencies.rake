require 'ceedling/constants'

namespace REFRESH_SYM do

  task TEST_SYM do
    @ceedling[:test_invoker].refresh_deep_dependencies
  end

end
