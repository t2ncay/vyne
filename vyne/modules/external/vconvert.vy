module vconvert;

# AN EXTERNAL VYNE MODULE TO CONVERT SCIENTIFIC UNITS WITH EASE


# ~~~Tempretaure Conversions~~~
fn :: vconvert celsiusToFahrenheit(c) {
    return c * (9.0 / 5.0) + 32.0;
}

fn :: vconvert celsiusToKelvin(c) {
    return c + 273.15;
}

fn :: vconvert fahrenheitToCelsius(f) {
    return (f - 32.0) * 5.0 / 9.0;
}

fn :: vconvert fahrenheitToKelvin(f) { 
    return (((f - 32) * 5) / 9) + 273.15;
}

fn :: vconvert kelvinToCelsius(k) {
    return k - 273.15;
}

fn :: vconvert kelvinToFahrenheit(f) {
    return ((k - 273.15) * 9)/5 + 32;
}

# ~~~Distance Conversions~~~

fn :: vconvert kmToMiles(km) {
    return km * 0.621371;
}

fn :: vconvert milesToKm(miles) {
    return miles * 1.60934;
}

fn :: vconvert kmToMeter(km) { 
    return km * 1000;
}

fn :: vconvert meterToKm(m) {
    return m / 1000;
}

fn :: vconvert meterToCm(m) {
    return m * 100;
}

fn  :: vconvert cmToMeter(cm) {
    return cm / 100;
}

fn :: vconvert cmToMm(cm) {
    return cm * 100;
}

fn :: vconvert mmToCm(mm) {
    return mm / 100;
}

# ~~~Time Conversions~~~

fn :: vconvert hoursToMin(h) {
    return h * 60;
}

fn :: vconvert minToHours(m) {
    return m / 60;
}

fn :: vconvert hoursToSec(h) {
    return h * 3600;
}

fn :: vconvert secToHours(s) {
    return s / 3600;
}

fn :: vconvert minToSec(m) {
    return m * 60;
}

fn :: vconvert secToMin(s) {
    return s / 60;
}

fn :: vconvert secToMs(s) {
    return s * 1000;
}

fn :: vconvert msToSec(ms) {
    return ms / 1000;
}

deploy vconvert;