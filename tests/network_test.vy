use extern "vnetwork.vy";

url = "https://httpbin.org/get/";
response = vnetwork.get(url);
out(response);