use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each(100) * 1 * blocks();

no_shuffle();
run_tests();

__DATA__

=== TEST 1: flush all
--- config
location /test.html {
            client_cache bdb:/usr/local/nginx/logs/pair.db;
            root           html;
            fastcgi_pass   127.0.0.1:9000;
            fastcgi_index  index.php;
            fastcgi_param  SCRIPT_FILENAME  $document_root/index.php;
            include        fastcgi_params;
}
--- response_headers
Content-Type: text/html
cache-control: max-age=3300
--- request
GET /test.html

=== TEST 2: Not set
--- server_config
    http {
            client_cache bdb:/usr/local/nginx/logs/pair.db;
}
--- response_headers
Cache-control:
--- request
GET /
