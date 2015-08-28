require 'mini_portile'

class PatchedMiniPortile < MiniPortile
  def configure
    return if configured?
    execute('autogen', %Q(sh autogen.sh)) unless File.exist?(File.join(work_path, "configure"))
    super
  end

  def download_file_http(url, full_path, count = 3)
    filename = File.basename(full_path)
    uri = URI.parse(url)

    if ENV['http_proxy']
      _, userinfo, p_host, p_port = URI.split(ENV['http_proxy'])
      proxy_user, proxy_pass = userinfo.split(/:/) if userinfo
      http = Net::HTTP.new(uri.host, uri.port, p_host, p_port, proxy_user, proxy_pass)
    else
      http = Net::HTTP.new(uri.host, uri.port)

      if URI::HTTPS === uri
        http.use_ssl = true
        http.verify_mode = OpenSSL::SSL::VERIFY_PEER

        store = OpenSSL::X509::Store.new

        # Auto-include system-provided certificates
        store.set_default_paths

        if ENV.has_key?("SSL_CERT_FILE") && File.exist?(ENV["SSL_CERT_FILE"])
          store.add_file ENV["SSL_CERT_FILE"]
        end

        http.cert_store = store
      end
    end
    if count == 3
      message "Downloading #{filename} "
    else
      message "\rDownloading #{filename} "
    end
    http.start do |h|
      h.request_get(uri.path, 'Accept-Encoding' => 'identity') do |response|
        case response
        when Net::HTTPNotFound
          output "404 - Not Found"
          return false

        when Net::HTTPClientError
          output "Error: Client Error: #{response.inspect}"
          return false

        when Net::HTTPRedirection
          raise "Too many redirections for the original URL, halting." if count <= 0
          url = response["location"]
          return download_file(url, full_path, count - 1)

        when Net::HTTPOK
          return with_tempfile(filename, full_path) do |temp_file|
            size = 0
            progress = 0
            total = response.header["Content-Length"].to_i
            response.read_body do |chunk|
              temp_file << chunk
              size += chunk.size
              new_progress = total.zero? ? 0 : (size * 100) / total
              unless new_progress == progress
                message "\r2Downloading %s (%3d%%) " % [filename, new_progress]
              end
              progress = new_progress
            end
            output
          end
        end
      end
    end
  end

  def download_file_ftp(uri, full_path)
    filename = File.basename(uri.path)
    with_tempfile(filename, full_path) do |temp_file|
      size = 0
      progress = 0
      Net::FTP.open(uri.host, uri.user, uri.password) do |ftp|
        ftp.passive = true
        ftp.login
        remote_dir = File.dirname(uri.path)
        ftp.chdir(remote_dir) unless remote_dir == '.'
        total = ftp.size(filename)
        ftp.getbinaryfile(filename, temp_file.path, 8192) do |chunk|
          # Ruby 1.8.7 already wrote the chunk into the file
          unless RUBY_VERSION < "1.9"
            temp_file << chunk
          end

          size += chunk.size
          new_progress = total.zero? ? 0 : (size * 100) / total
          unless new_progress == progress
            message "\rDownloading %s (%3d%%) " % [filename, new_progress]
          end
          progress = new_progress
        end
      end
      output
    end
  end
end
