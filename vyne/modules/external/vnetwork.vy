module vcore;
module vnetwork;

fn :: vnetwork get(url) {
    return vcore.http_get(url);
}

deploy vnetwork;