require 'gctools/oobgc'

module GC
  module OOB
    module UnicornMiddleware
      def self.new(app)
        ObjectSpace.each_object(Unicorn::HttpServer) do |s|
          s.extend(self)
        end
        app # pretend to be Rack middleware since it was in the past
      end
      def process_client(client)
        super(client) # Unicorn::HttpServer#process_client
        GC::OOB.run
      end
    end
  end
end
