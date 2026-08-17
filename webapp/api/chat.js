export default async function handler(req, res) {
    if (req.method !== 'POST') {
        return res.status(405).json({ error: 'Method not allowed' });
    }

    const { question } = req.body;
    if (!question) {
        return res.status(400).json({ error: 'Question required' });
    }

    try {
        const response = await fetch('https://api.groq.com/openai/v1/chat/completions', {
            method: 'POST',
            headers: {
                'Authorization': `Bearer ${process.env.GROQ_API_KEY}`,
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                model: 'llama-3.1-8b-instant',
                messages: [
                    { 
                        role: 'system', 
                        content: 'You are OPRUSS AI. Answer based on air quality sensor data provided in context. Be concise, helpful, and specific. If no data is provided, say you need sensor readings.' 
                    },
                    { role: 'user', content: question }
                ],
                max_tokens: 500,
                temperature: 0.7
            })
        });

        const data = await response.json();
        
        if (data.choices?.[0]?.message?.content) {
            return res.json({ answer: data.choices[0].message.content });
        }
        
        return res.json({ answer: 'AI service unavailable. Try again later.' });
    } catch (error) {
        return res.json({ answer: 'Service temporarily unavailable.' });
    }
}