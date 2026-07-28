import sys
from google import genai

client = genai.Client()

print("Gemini CLI Active. Type 'exit' to quit.\n")
while True:
    try:
        user_input = input("You: ")
        if user_input.lower() in ['exit', 'quit']:
            break
        
        response = client.models.generate_content(
            model='gemini-2.5-flash-001', 
            contents=user_input
        )
        print(f"\nGemini:\n{response.text}\n")
    except KeyboardInterrupt:
        break
