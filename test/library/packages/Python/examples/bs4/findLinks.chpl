use Python;
use List;


const html =
' \
<html> \
  <head> \
    <title>Sample HTML</title> \
  </head> \
  <body> \
    <h1>Sample Page</h1> \
    <p>This is a <a href="http://example.com">link</a> to example.com.</p> \
    <p>Here is another <a href="http://chapel-lang.org">link</a> to the Chapel website.</p> \
    <p>And one more <a href="http://github.com">link</a> to GitHub.</p> \
  </body> \
</html> \
';

proc main() {

  var interp = new Interpreter();
  var mod = interp.importModule("bs4");

  var cls = mod.get("BeautifulSoup");
  var soup = cls(html, 'html.parser');

  var res: list(owned Value?);
  res = soup.call(res.type, "find_all", "a");
  for c in res {
    var linkText = c!.get(string, "text");
    var linkUrl = c!.call(string, "__getitem__", "href");
    writeln(linkText, ": ", linkUrl);
  }
}

