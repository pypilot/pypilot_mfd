void buzzer_buzz()
{
    // buzzer (backlight uses channel 0)
    ledcSetup(1, 4000, 10);
    ledcAttachPin(4, 1);
    ledcWrite(1, 512);
    delay(100);
    ledcWrite(1, 0);
    letcDetachPin(4);
}
