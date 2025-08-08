// ESP32 Control Panel Service Worker
// Provides offline functionality and caching for PWA

const CACHE_NAME = 'esp32-control-panel-v2.3.0';
const OFFLINE_URL = '/offline.html';

// Files to cache for offline functionality
const STATIC_CACHE_URLS = [
  '/',
  '/style.css',
  '/app.js',
  '/manifest.json',
  OFFLINE_URL
];

// Install event - cache static resources
self.addEventListener('install', (event) => {
  console.log('[ServiceWorker] Install event');
  
  event.waitUntil(
    (async () => {
      const cache = await caches.open(CACHE_NAME);
      console.log('[ServiceWorker] Caching static resources');
      try {
        await cache.addAll(STATIC_CACHE_URLS);
      } catch (error) {
        console.error('[ServiceWorker] Failed to cache resources:', error);
      }
    })()
  );
  
  // Force the waiting service worker to become the active service worker
  self.skipWaiting();
});

// Activate event - clean up old caches
self.addEventListener('activate', (event) => {
  console.log('[ServiceWorker] Activate event');
  
  event.waitUntil(
    (async () => {
      // Clean up old caches
      const cacheNames = await caches.keys();
      await Promise.all(
        cacheNames.map((cacheName) => {
          if (cacheName !== CACHE_NAME) {
            console.log('[ServiceWorker] Deleting old cache:', cacheName);
            return caches.delete(cacheName);
          }
        })
      );
    })()
  );
  
  // Claim all clients
  self.clients.claim();
});

// Fetch event - serve cached content when offline
self.addEventListener('fetch', (event) => {
  const { request } = event;
  const url = new URL(request.url);
  
  // Only handle requests to our origin
  if (url.origin !== location.origin) return;
  
  event.respondWith(
    (async () => {
      try {
        // Try to fetch from network first (for live data)
        const networkResponse = await fetch(request);
        
        // Cache successful responses (except for API endpoints)
        if (networkResponse.status === 200 && !isApiEndpoint(url.pathname)) {
          const cache = await caches.open(CACHE_NAME);
          cache.put(request, networkResponse.clone());
        }
        
        return networkResponse;
      } catch (error) {
        console.log('[ServiceWorker] Network fetch failed, trying cache:', request.url);
        
        // Network failed, try to serve from cache
        const cachedResponse = await caches.match(request);
        if (cachedResponse) {
          return cachedResponse;
        }
        
        // If requesting an HTML page and not in cache, serve offline page
        if (request.destination === 'document') {
          return caches.match(OFFLINE_URL);
        }
        
        // For other resources, return a basic response
        return new Response('Offline - Resource not available', {
          status: 503,
          statusText: 'Service Unavailable',
          headers: new Headers({
            'Content-Type': 'text/plain',
          }),
        });
      }
    })()
  );
});

// Helper function to check if URL is an API endpoint
function isApiEndpoint(pathname) {
  const apiPaths = ['/status', '/reboot', '/alerts/', '/telnet/'];
  return apiPaths.some(path => pathname.startsWith(path));
}

// Background sync for when connection is restored
self.addEventListener('sync', (event) => {
  if (event.tag === 'background-sync') {
    console.log('[ServiceWorker] Background sync triggered');
    event.waitUntil(
      // Could implement queued actions here
      Promise.resolve()
    );
  }
});

// Push notifications (for future enhancement)
self.addEventListener('push', (event) => {
  if (event.data) {
    const data = event.data.json();
    const options = {
      body: data.body,
      icon: '/icon-192.png',
      badge: '/icon-192.png',
      vibrate: [100, 50, 100],
      data: {
        dateOfArrival: Date.now(),
        primaryKey: data.primaryKey || 1
      },
      actions: [
        {
          action: 'explore',
          title: 'View Device',
          icon: '/icon-192.png'
        },
        {
          action: 'close',
          title: 'Close',
          icon: '/icon-192.png'
        }
      ]
    };
    
    event.waitUntil(
      self.registration.showNotification(data.title, options)
    );
  }
});

// Notification click handler
self.addEventListener('notificationclick', (event) => {
  event.notification.close();
  
  if (event.action === 'explore') {
    event.waitUntil(
      clients.openWindow('/')
    );
  }
});

// Message handler for communication with main app
self.addEventListener('message', (event) => {
  if (event.data && event.data.type === 'SKIP_WAITING') {
    self.skipWaiting();
  }
});

console.log('[ServiceWorker] Service Worker loaded');
