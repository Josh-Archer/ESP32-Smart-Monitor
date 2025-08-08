FROM nginx:alpine
# Copy static web assets
COPY web/ /usr/share/nginx/html/
# Provide custom nginx config for gzip and caching
COPY k8s/nginx.conf /etc/nginx/conf.d/default.conf
