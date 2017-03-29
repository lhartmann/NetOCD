//         SSI tag name (8 chars maximum)
//         |         Printf format string
//         |         |     Value or expression
SSI_TAGVAL(uptime,   "%d", xTaskGetTickCount() * portTICK_PERIOD_MS / 1000)
SSI_TAGVAL(heap,     "%d", (int) xPortGetFreeHeapSize())
SSI_TAGVAL(led,      "%s", (GPIO.OUT & BIT(LED_PIN)) ? "Off" : "On")
SSI_TAGVAL(hitcount, "%d", hitcount())
//SSI_TAGVAL(LED1In,   "<input type=\"checkbox\" name=\"LED1On\"%s>", i32Led1State ? " checked" : "")

// Functions that add complex output, should have the following prototype:
// void FUNCTION(char *buffer, int maxlen);
//
//         SSI tag name (8 chars maximum)
//         |         Name of a C function
SSI_TAGFCN(funcao1,  funcao1)
