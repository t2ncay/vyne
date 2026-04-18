ruleset { dynamic_casting };

use lib "vml.vy";
use lib "vcolors.vy";
use lib "vlinalg.vy";

module vcore;
module vfs;

out(vcolors.cyan("=== VyneNet Grade Predictor Training ==="));

# 1. Dataset Hazırlığı (Normalized: 0.0 - 1.0)
# Sütunlar: [Midterm 1, Midterm 2, Attendance %]
training_inputs = vlinalg.Types.Matrix(8, 3, [
    [0.95, 0.90, 1.00], # Əlaçı, hər şey super
    [0.10, 0.15, 0.20], # Dərsi tamamilə buraxıb
    [0.60, 0.65, 0.90], # Orta tələbə, davamiyyət yaxşı
    [0.40, 0.45, 0.50], # Sərhəddə olan tələbə
    [0.80, 0.30, 0.95], # MT1 yaxşı, MT2-də bəxti gətirməyən
    [0.25, 0.85, 0.80], # MT1 zəif, amma sonradan toparlayan
    [0.55, 0.55, 1.00], # Stabil orta, heç bir dərsi qaçırtmır
    [0.70, 0.70, 0.40]  # İmtahanlar yaxşı, amma kəsilmə riski (davamiyyət)
]);

# Hədəf: Finalda aldığı real nəticə
# 0.5-dən yuxarı PASS (S), aşağı FAIL (U) sayılacaq
training_targets = vlinalg.Types.Matrix(8, 1, [
    [0.92], # Pass
    [0.10], # Fail
    [0.68], # Pass
    [0.45], # Fail (borderline)
    [0.55], # Pass (MT1 sayəsində)
    [0.75], # Pass (Great comeback!)
    [0.60], # Pass
    [0.35]  # Fail (Attendance killed it)
]);

# 2. Modelin Yaradılması (3 input, 6 hidden neyron, 1 output)
# METU standartlarına uyğun 0.05 learning rate qoyuruq
grader_net = vml.create_network(3, 6, 1, 0.05);

# 3. Training Loop
epochs = 50000;
out("Training for " + string(epochs) + " epochs...");

through i :: 1..epochs -> loop {
    grader_net = vml.train_step(grader_net, training_inputs, training_targets);
    
    if i % 5000 == 0 {
        out("Check: Epoch " + string(i) + " completed.");
    }
};

out(vcolors.green("Model trained! Saving to disk..."));
#vml.save_model(grader_net, "metu_grader_v1");

# 4. Real Ssenari Testi (İnference)
out("\n" + vcolors.yellow("--- Testing with New Student Data ---"));

# Ssenari: Tələbə MT1: 70, MT2: 65, Attendance: 60% (Kəsilməlidir!)
test_data = [0.91, 0.65, 1.0];
test_student = vlinalg.Types.Matrix(1, 3, [test_data]);

# 70% Qaydası (Hard Logic)
attendance = test_data[2];

if attendance < 0.01 {
    out(vcolors.red("Status: NA (Non-Attendance)"));
    out("Result: Student fails automatically due to 70% rule.");
} else {
    # Əgər davamiyyət qaydasını keçibsə, onda Neural Network-ə sorğu atırıq
    prediction = vml.predict(grader_net, test_student);
    predicted_grade = prediction.data[0][0] * 100;
    
    out("Predicted Final Score: " + string(predicted_grade));

    if predicted_grade >= 50 {
        out(vcolors.green("Prediction: Student will PASS (S)"));
    } else {
        out(vcolors.red("Prediction: Student will FAIL (U)"));
    }
}