require 'ceedling/constants'

namespace REFRESH_SYM do

  task RELEASE_SYM do
    @ceedling[:release_invoker].refresh_c_deep_dependencies
  end

end
