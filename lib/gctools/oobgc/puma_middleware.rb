require 'gctools/oobgc'

module GC
  module OOB
    class PumaMiddleware
      def initialize(app, name = nil)
        @app = app
      end
      def call(env)
        status, header, body = @app.call(env)
        env['rack.after_reply'] << lambda { GC::OOB.run }
        [status, header, body]
      end
    end
  end
end
