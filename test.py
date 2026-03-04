import urllib.request
import urllib.error

url = "https://ky28nlq70u99.dssldrf.net/PINGME"

try:
    # Adding a User-Agent header is good practice as some 
    # servers block the default "Python-urllib" identifier.
    headers = {'User-Agent': 'Mozilla/5.0'}
    req = urllib.request.Request(url, headers=headers)

    # Context manager ensures the connection is closed automatically
    with urllib.request.urlopen(req, timeout=10) as response:
        status = response.getcode()
        content = response.read().decode('utf-8')
        print(f"Success! Status Code: {status}")
        print(f"Response: {content}")

except urllib.error.HTTPError as e:
    print(f"HTTP Error: {e.code} - {e.reason}")
except urllib.error.URLError as e:
    print(f"Connection Error: {e.reason}")
except Exception as e:
    print(f"An unexpected error occurred: {e}")
