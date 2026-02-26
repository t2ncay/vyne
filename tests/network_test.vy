module vcore;

url = "https://httpbin.org/get/";
response = vcore.http_get(url);
out(response);