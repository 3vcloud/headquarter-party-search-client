const whitelisted_ips  = [
    '127.0.0.1'
];


/**
 *
 * @param request {Request|WebSocket}
 * @returns {string}
 */
export function get_ip_from_request(request) {
    let ip = '';

    if(!request)
        return ip;
    if(request._socket)
        ip = request._socket.remoteAddress;
    if(request.socket)
        ip = request.socket.remoteAddress || ip;
    if(request.headers)
        ip = request.headers['x-forwarded-for'] || ip;
    if(ip === '::1')
        ip = '127.0.0.1';
    return ip.split(':').pop();
}

/**
 *
 * @param request {Request|WebSocket}
 * @returns {boolean}
 */
export function is_whitelisted(request) {
    return whitelisted_ips.includes(get_ip_from_request(request));
}